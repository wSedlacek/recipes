/* gr-recipes-page.c:
 *
 * Copyright (C) 2016 Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gr-recipe.h"
#include "gr-recipes-page.h"
#include "gr-recipe-tile.h"
#include "gr-category-tile.h"
#include "gr-chef-tile.h"
#include "gr-recipe-store.h"
#include "gr-app.h"
#include "gr-utils.h"
#include "gr-window.h"


struct _GrRecipesPage
{
        GtkBox parent_instance;

        GtkWidget *today_box;
        GtkWidget *pick_box;
        GtkWidget *diet_box;
        GtkWidget *chefs_box;
        GtkWidget *favorites_box;
};

G_DEFINE_TYPE (GrRecipesPage, gr_recipes_page, GTK_TYPE_BOX)

static void populate_static (GrRecipesPage *page);
static void populate_recipes_from_store (GrRecipesPage *page);
static void populate_chefs_from_store (GrRecipesPage *page);
static void connect_store_signals (GrRecipesPage *page);

static void
recipes_page_finalize (GObject *object)
{
        G_OBJECT_CLASS (gr_recipes_page_parent_class)->finalize (object);
}

static void
gr_recipes_page_init (GrRecipesPage *page)
{
        gtk_widget_set_has_window (GTK_WIDGET (page), FALSE);
        gtk_widget_init_template (GTK_WIDGET (page));

        populate_static (page);
        populate_recipes_from_store (page);
        populate_chefs_from_store (page);
        connect_store_signals (page);
}

static void
gr_recipes_page_class_init (GrRecipesPageClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GParamSpec *pspec;

        object_class->finalize = recipes_page_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Recipes/gr-recipes-page.ui");

        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrRecipesPage, today_box);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrRecipesPage, pick_box);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrRecipesPage, diet_box);
        gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (klass), GrRecipesPage, chefs_box);
}

GtkWidget *
gr_recipes_page_new (void)
{
        GrRecipesPage *page;

        page = g_object_new (GR_TYPE_RECIPES_PAGE, NULL);

        return GTK_WIDGET (page);
}

static void
category_clicked (GrCategoryTile *tile,
                  GrRecipesPage  *page)
{
        GtkWidget *window;
        GrDiets diet;
        const char *name;
        const char *label;

        window = gtk_widget_get_ancestor (GTK_WIDGET (tile), GR_TYPE_WINDOW);

        diet = gr_category_tile_get_diet (tile);
        name = gr_category_tile_get_category (tile);
        label = gr_category_tile_get_label (tile);

        if (diet)
                gr_window_show_diet (GR_WINDOW (window), label, diet);
        else if (strcmp (name, "favorites") == 0)
                gr_window_show_favorites (GR_WINDOW (window));
        else if (strcmp (name, "mine") == 0)
                gr_window_show_myself (GR_WINDOW (window));
}

static void
populate_static (GrRecipesPage *self)
{
        int i;
	GrDiets diets[5] = {
                GR_DIET_GLUTEN_FREE,
                GR_DIET_NUT_FREE,
                GR_DIET_VEGAN,
                GR_DIET_VEGETARIAN,
                GR_DIET_MILK_FREE
        };
        GtkWidget *tile;
        GrRecipeStore *store;

        tile = gr_category_tile_new_with_label ("mine", _("My recipes"));
        gtk_widget_show (tile);
        gtk_container_add (GTK_CONTAINER (self->diet_box), tile);
        g_signal_connect (tile, "clicked", G_CALLBACK (category_clicked), self);

        tile = gr_category_tile_new_with_label ("favorites", _("Favorites"));
        gtk_widget_show (tile);
        gtk_container_add (GTK_CONTAINER (self->diet_box), tile);
        g_signal_connect (tile, "clicked", G_CALLBACK (category_clicked), self);

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        for (i = 0; i < 5; i++) {
                if (!gr_recipe_store_has_diet (store, diets[i]))
                        continue;

                tile = gr_category_tile_new (diets[i]);
                gtk_widget_show (tile);
                gtk_container_add (GTK_CONTAINER (self->diet_box), tile);
                g_signal_connect (tile, "clicked", G_CALLBACK (category_clicked), self);
        }
}

static void
populate_recipes_from_store (GrRecipesPage *self)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int i;

        container_remove_all (GTK_CONTAINER (self->today_box));
        container_remove_all (GTK_CONTAINER (self->pick_box));

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        keys = gr_recipe_store_get_recipe_keys (store, &length);
        for (i = 0; i < length; i++) {
                g_autoptr(GrRecipe) recipe = NULL;
                GtkWidget *tile;

                recipe = gr_recipe_store_get (store, keys[i]);

                if (gr_recipe_store_is_todays (store, recipe)) {
                        tile = gr_recipe_tile_new (recipe);
                        gtk_widget_show (tile);
                        gtk_container_add (GTK_CONTAINER (self->today_box), tile);
                }
                else if (gr_recipe_store_is_pick (store, recipe)) {
                        tile = gr_recipe_tile_new (recipe);
                        gtk_widget_show (tile);
                        gtk_container_add (GTK_CONTAINER (self->pick_box), tile);
                }


        }
}

static void
populate_chefs_from_store (GrRecipesPage *self)
{
        GrRecipeStore *store;
        g_autofree char **keys = NULL;
        guint length;
        int i;

        container_remove_all (GTK_CONTAINER (self->chefs_box));

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        keys = gr_recipe_store_get_chef_keys (store, &length);
        for (i = 0; i < length; i++) {
                g_autoptr(GrChef) chef = NULL;

                chef = gr_recipe_store_get_chef (store, keys[i]);

		if (gr_recipe_store_chef_is_featured (store, chef)) {
			GtkWidget *tile;
			tile = gr_chef_tile_new (chef);
			gtk_widget_show (tile);
			gtk_container_add (GTK_CONTAINER (self->chefs_box), tile);
		}
	}
}

static void
connect_store_signals (GrRecipesPage *page)
{
        GrRecipeStore *store;

        store = gr_app_get_recipe_store (GR_APP (g_application_get_default ()));

        /* FIXME: inefficient */
        g_signal_connect_swapped (store, "recipe-added", G_CALLBACK (populate_recipes_from_store), page);
        g_signal_connect_swapped (store, "recipe-removed", G_CALLBACK (populate_recipes_from_store), page);
        g_signal_connect_swapped (store, "recipe-changed", G_CALLBACK (populate_recipes_from_store), page);
        g_signal_connect_swapped (store, "chefs-changed", G_CALLBACK (populate_chefs_from_store), page);
}
